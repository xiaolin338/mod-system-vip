DROP TABLE IF EXISTS `account_vip`;

CREATE TABLE `account_vip` (
  `id` int(11) NOT NULL DEFAULT '0',
  `subscription_date` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;